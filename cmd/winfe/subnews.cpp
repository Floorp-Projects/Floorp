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

#include "property.h"
#include "styles.h"

#include "helper.h"
#include "display.h"
#include "dialog.h"

#include "secnav.h"
#include "custom.h"
#include "cxabstra.h"

#include "msgcom.h"
#include "subnews.h"
#include "wfemsg.h"
#include "mnprefs.h"
#include "nethelp.h"	// help
#include "xp_help.h"	 
#include "prefapi.h"	 
#include "thrdfrm.h"	 
#include "fldrfrm.h"	 

#ifdef _DEBUG
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

#define LEVEL_ROOT		0
#define LEVEL_1			1
#define SUBSCRIBE_VISIBLE_TIMER   999

extern "C" BOOL IsNumeric(char* pStr);
extern "C" void HelperInitFonts( HDC hdc , HFONT *phFont, HFONT *phBoldFont);
extern "C" MSG_Host *DoAddNewsServer(CWnd* pParent, int nFromWhere);
extern "C" MSG_Host *DoAddIMAPServer(CWnd* pParent, char* pServerName, BOOL bImap);


extern "C" XP_Bool FE_CreateSubscribePaneOnHost(MSG_Master* master, 
           MWContext* parentContext, MSG_Host* host)
{
	CGenericFrame *pFrame = theApp.m_pFrameList;
	while (pFrame) {
		if (pFrame->IsKindOf(RUNTIME_CLASS(C3PaneMailFrame))) {
			C3PaneMailFrame *pThreadFrame = DYNAMIC_DOWNCAST(C3PaneMailFrame, pFrame);
			pThreadFrame->SendMessage(WM_COMMAND, ID_FILE_SUBSCRIBE, (LPARAM)host);
			return TRUE;
		}
		pFrame = pFrame->m_pNext;
	}
	pFrame = theApp.m_pFrameList;
	while (pFrame) {
		if (pFrame->IsKindOf(RUNTIME_CLASS(CFolderFrame))) {
			CFolderFrame *pFolderFrame = DYNAMIC_DOWNCAST(CFolderFrame, pFrame);
			pFolderFrame->SendMessage(WM_COMMAND, ID_FILE_SUBSCRIBE, (LPARAM)host);
			return TRUE;
		}
		pFrame = pFrame->m_pNext;
	}
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CServersCombo  

#define SERVER_BUTTONWIDTH	24
#define SERVER_BUTTONHEIGHT	16

CServersCombo::CServersCombo()
{
	m_bStaticCtl = FALSE;

	m_hFont = NULL;
	m_hBoldFont = NULL;
    ApiApiPtr(api);

    m_pIImageUnk = api->CreateClassInstance(APICLASS_IMAGEMAP);
	ASSERT(m_pIImageUnk);
    if (m_pIImageUnk) {
        m_pIImageUnk->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIImageMap);
        ASSERT(m_pIImageMap);
        m_pIImageMap->Initialize(IDB_MAILNEWS,16,16);
    }
}

CServersCombo::~CServersCombo()
{
	if (m_hFont)
		theApp.ReleaseAppFont(m_hFont);
	if (m_hBoldFont)
		theApp.ReleaseAppFont(m_hBoldFont);
}

void CServersCombo::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	HDC hDC = lpDrawItemStruct->hDC;
	RECT rcItem = lpDrawItemStruct->rcItem;
	RECT rcTemp = rcItem;
	RECT rcText;
	DWORD dwItemData = lpDrawItemStruct->itemData;
	HBRUSH hBrushWindow = ::CreateSolidBrush( GetSysColor( COLOR_WINDOW ) );
	HBRUSH hBrushHigh = ::CreateSolidBrush( GetSysColor( COLOR_HIGHLIGHT ) );
	HBRUSH hBrushFill = NULL;

	if ( !m_hFont ) {
		HelperInitFonts( hDC, &m_hFont, &m_hBoldFont);
	}

	HFONT hOldFont = NULL;
	if ( m_hFont ) {
		hOldFont = (HFONT) ::SelectObject( hDC,(HFONT)&m_hFont );
	}

	if ( lpDrawItemStruct->itemState & ODS_SELECTED ) {
		hBrushFill = hBrushHigh;
		::SetBkColor( hDC, GetSysColor( COLOR_HIGHLIGHT ) );
		::SetTextColor( hDC, GetSysColor( COLOR_HIGHLIGHTTEXT ) );
		m_pIImageMap->UseHighlight( );
	} else {
		hBrushFill = hBrushWindow;
		::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
		m_pIImageMap->UseNormal( );
	}

	VERIFY(::FillRect( hDC, &rcItem, hBrushFill ));

	if ( lpDrawItemStruct->itemID != -1 &&  dwItemData )
	{
		int idxImage;

		MSG_Host *pServer = (MSG_Host *) lpDrawItemStruct->itemData;

		if (MSG_IsNewsHost(pServer))
			idxImage = IDX_NEWSHOST;
		else
			idxImage = IDX_REMOTEMAIL;

		BOOL bStatic = FALSE;
#ifdef _WIN32
	if ( sysInfo.m_bWin4 )
		bStatic = ( lpDrawItemStruct->itemState & ODS_COMBOBOXEDIT ) ? TRUE : FALSE;
	else 
#endif
		bStatic = m_bStaticCtl;

		//Draw the news bitmap
		int iIndent = 4;

        m_pIImageMap->DrawImage( idxImage, iIndent, rcItem.top, hDC, FALSE );

		//Draw the text
		LPCTSTR name = (LPCTSTR) MSG_GetHostUIName(pServer);

		int iWidth = rcTemp.right - rcTemp.left;

		rcTemp = rcItem;
		rcText = rcItem;
		rcTemp.left = iIndent + 20;
		rcTemp.right = rcTemp.left + iWidth + 4;

		VERIFY(::FillRect( hDC, &rcTemp, hBrushFill ));
		rcText.left = rcTemp.left + 2;
		rcText.right = rcTemp.right - 2;
		::DrawText( hDC, name, -1, &rcText, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );

		//Draw the focus
		if ( lpDrawItemStruct->itemAction & ODA_FOCUS && 
			 lpDrawItemStruct->itemState & ODS_SELECTED )
		{
			::DrawFocusRect( hDC, &rcItem);
		}	
	}

	if ( hBrushHigh ) 
		VERIFY( ::DeleteObject( hBrushHigh ));
	if ( hBrushWindow ) 
		VERIFY( ::DeleteObject( hBrushWindow ));

	if ( hOldFont )
		::SelectObject( hDC, hOldFont );
}

BEGIN_MESSAGE_MAP( CServersCombo, CComboBox )
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSubscribeCX
CSubscribeCX::CSubscribeCX(CNetscapePropertySheet *pSheet)  
: CStubsCX(NewsCX, MWContextNews)
{
	m_pSheet = (CSubscribePropertySheet*)pSheet;
	m_lPercent = 0;
	m_bAnimated = FALSE;
}

void CSubscribeCX::SetProgressBarPercent(MWContext *pContext, int32 lPercent )  
{
	//	Ensure the safety of the value.

	lPercent = lPercent < 0 ? 0 : ( lPercent > 100 ? 100 : lPercent );

	if ( m_lPercent == lPercent )  
		return;

	m_lPercent = lPercent;
	if (m_pSheet)  
		m_pSheet->SetProgressBarPercent(lPercent);
}

void CSubscribeCX::Progress(MWContext *pContext, const char *pMessage)
{
	m_csProgress = pMessage;
	if (m_pSheet)  
		m_pSheet->SetStatusText(pMessage);
}

int32 CSubscribeCX::QueryProgressPercent()	
{
	return m_lPercent;
}

void CSubscribeCX::AllConnectionsComplete(MWContext *pContext)    
{
    //  Call the base.
    CStubsCX::AllConnectionsComplete(pContext);

	//	Also, we can clear the progress bar now.
	m_lPercent = 0;
	if ( m_pSheet ) 
	{
		m_pSheet->SetProgressBarPercent(m_lPercent);
		m_pSheet->AllConnectionsComplete(pContext);
	}
#ifdef _WIN32
    if (m_pSheet) 
	{
		m_pSheet->SendMessage(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		m_pSheet->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
#endif
}

void CSubscribeCX::UpdateStopState( MWContext *pContext )
{
#ifdef _WIN32
    if (m_pSheet) 
	{
		m_pSheet->SendMessage(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
		m_pSheet->SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////
// CSubscribeList
STDMETHODIMP CSubscribeList::QueryInterface(REFIID refiid, LPVOID * ppv)
{
	*ppv = NULL;
	if (IsEqualIID(refiid,IID_IUnknown))
   		*ppv = (LPUNKNOWN) this;
	else if (IsEqualIID(refiid,IID_IMsgList))
   		*ppv = (LPMSGLIST) this;

	if (*ppv != NULL) {
   		((LPUNKNOWN) *ppv)->AddRef();
		return NOERROR;
	}
            
	return ResultFromScode(E_NOINTERFACE);
}

STDMETHODIMP_(ULONG) CSubscribeList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) CSubscribeList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void CSubscribeList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pSubscribePage) 
		m_pSubscribePage->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
}

void CSubscribeList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pSubscribePage) 
		m_pSubscribePage->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
}

void CSubscribeList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void CSubscribeList::SelectItem( MSG_Pane* pane, int item )
{
}

/////////////////////////////////////////////////////////////////////////////
// CNewsgroupsOutlinerParent

BEGIN_MESSAGE_MAP(CNewsgroupsOutlinerParent, COutlinerParent)
END_MESSAGE_MAP()

CNewsgroupsOutlinerParent::CNewsgroupsOutlinerParent()
{
}


CNewsgroupsOutlinerParent::~CNewsgroupsOutlinerParent()
{
 
}

// Draw Column text and Sort indicator
BOOL CNewsgroupsOutlinerParent::RenderData (int idColumn, CRect & rect, CDC &dc, const char * text)
{
	CNewsgroupsOutliner* pOutliner = (CNewsgroupsOutliner*) m_pOutliner;

	// Draw Text String
	if (idColumn == ID_COLNEWS_SUBSCRIBE)
		dc.DrawText(text,  _tcslen(text), &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	else
		dc.DrawText(text,  _tcslen(text), &rect, DT_SINGLELINE | DT_VCENTER);

	// Draw Sort Indicator
	MSG_COMMAND_CHECK_STATE sortType = 
		pOutliner->m_attribSortBy == idColumn ? MSG_Checked : MSG_Unchecked;

	int idxImage = pOutliner->m_bSortAscending ? IDX_SORTINDICATORDOWN : IDX_SORTINDICATORUP;
	CSize cs = dc.GetTextExtent(text, _tcslen(text));

	if (idColumn == pOutliner->m_attribSortBy && cs.cx + 22 <= rect.Width()) {
        m_pIImage->DrawTransImage( idxImage,
								   rect.left + 8 + cs.cx,
								   (rect.top + rect.bottom) / 2 - 4,
								   &dc );
	}

    return TRUE;
}


COutliner * CNewsgroupsOutlinerParent::GetOutliner ( void )
{
    return new CNewsgroupsOutliner;
}

void CNewsgroupsOutlinerParent::CreateColumns ( void )
{
	CString text, newName; 
	CRect colRect;
	int col1, col2, col3, nPos;
	int pos1, pos2, pos3;

	if (m_pSheet->nameWidth == -1)
	{
		CDC* pDC = GetDC();
		text.LoadString(IDS_SUBSCRIBE);
		CSize textSize = pDC->GetTextExtent(LPCTSTR(text), text.GetLength());
		ReleaseDC(pDC);
			 
		GetClientRect(colRect);	 
		col2 = textSize.cx + 10;
		col3 = col2;
		int nScrollWidth = ::GetSystemMetrics(SM_CXVSCROLL);
		col1 = (colRect.right - colRect.left - nScrollWidth) - (col2 + col3);
		m_pSheet->nameWidth = col1;
		m_pSheet->subscribeWidth  = col2;
		m_pSheet->postWidth = col3;
		m_pSheet->namePos = 0;
		m_pSheet->subscribePos = 1;
		m_pSheet->postPos = 2;
	}
	else
	{
		col1 = m_pSheet->nameWidth;
		col2 = m_pSheet->subscribeWidth;
		col3 = m_pSheet->postWidth;
		pos1 = m_pSheet->namePos;
		pos2 = m_pSheet->subscribePos;
		pos3 = m_pSheet->postPos;
	}

	text.LoadString(IDS_NEWSGROUP_NAME);
	m_pOutliner->AddColumn (text, ID_COLNEWS_NAME,		col1, colRect.right, 
							ColumnVariable, col1, FALSE);
	text.LoadString(IDS_SUBSCRIBE);	
	nPos = text.Find('&');
	if (nPos >= 0)
		newName = text.Left(nPos) + text.Right(text.GetLength() - nPos - 1);
	else
		newName = text;	
    m_pOutliner->AddColumn (newName, ID_COLNEWS_SUBSCRIBE,	col2, colRect.right, 
							ColumnVariable, col2, FALSE);
	text.LoadString(IDS_POSTINGS);
    m_pOutliner->AddColumn (text, ID_COLNEWS_POSTINGS,	col3, colRect.right, 
							ColumnVariable, col3, FALSE); 
 
	m_pOutliner->SetColumnPos( ID_COLNEWS_NAME, pos1 );
	m_pOutliner->SetColumnPos( ID_COLNEWS_SUBSCRIBE, pos2 );
	m_pOutliner->SetColumnPos( ID_COLNEWS_POSTINGS, pos3 );

	m_pOutliner->SetImageColumn( ID_COLNEWS_NAME );
}

//////////////////////////////////////////////////////////////////////////////
// CNewsgroupsOutliner

CNewsgroupsOutliner::CNewsgroupsOutliner()
{
	ApiApiPtr(api);
    m_pUnkUserImage = api->CreateClassInstance(APICLASS_IMAGEMAP);
    if (m_pUnkUserImage) {
        m_pUnkUserImage->QueryInterface(IID_IImageMap,(LPVOID*)&m_pIUserImage);
        ASSERT(m_pIUserImage);
        m_pIUserImage->Initialize(IDB_MAILNEWS,16,16);
    }

	m_pAncestor = NULL;
    m_pszExtraText = new char[256];
	m_pPage = NULL;
}

CNewsgroupsOutliner::~CNewsgroupsOutliner ( )
{
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	delete [] m_pAncestor;
	delete [] m_pszExtraText;
}

int CNewsgroupsOutliner::GetDepth( int iLine )
{
	MSG_GroupNameLine groupLine;
	MSG_GetGroupNameLineByIndex(m_pPane, (MSG_ViewIndex) iLine, 1, &groupLine );

	return groupLine.level;
}

int CNewsgroupsOutliner::GetNumChildren( int iLine )
{
	MSG_GroupNameLine groupLine;
	MSG_GetGroupNameLineByIndex(m_pPane, (MSG_ViewIndex) iLine, 1, &groupLine );

	return groupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN ? 1 : 0;
}

BOOL CNewsgroupsOutliner::IsCollapsed( int iLine )
{
	MSG_GroupNameLine groupLine;
	MSG_GetGroupNameLineByIndex(m_pPane, (MSG_ViewIndex) iLine, 1, &groupLine );

	return (groupLine.flags & MSG_GROUPNAME_FLAG_ELIDED) ? TRUE : FALSE;
}

BOOL CNewsgroupsOutliner::RenderData(UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
    if (iColumn != ID_COLNEWS_SUBSCRIBE)
		return FALSE;

	MSG_Host* pHost = ((CSubscribePropertySheet*)(GetPage()->GetParent()))->GetHost();
	if ((m_GroupLine.flags & MSG_GROUPNAME_FLAG_HASCHILDREN) && MSG_IsNewsHost(pHost))
		return TRUE;

	int idxImage = -1;

	if (m_GroupLine.flags & MSG_GROUPNAME_FLAG_SUBSCRIBED) 
		idxImage = IDX_CHECKMARK; 
	else
		idxImage = IDX_CHECKBOX; 

	m_pIUserImage->DrawImage(idxImage, rect.left + ( ( rect.Width ( ) - 16 ) / 2 ),
		                     rect.top, &dc, FALSE);
	return TRUE;
}


int CNewsgroupsOutliner::TranslateIcon(void * pLineData)
{
    ASSERT(pLineData);
    MSG_GroupNameLine *pGroup = (MSG_GroupNameLine*)pLineData;
	if (pGroup->flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
	{
		if (pGroup->flags & MSG_GROUPNAME_FLAG_ELIDED) 
			return IDX_MAILFOLDERCLOSED;
		else 
			return IDX_MAILFOLDEROPEN;
	}
	else 
	{
		if (pGroup->flags & MSG_GROUPNAME_FLAG_NEW_GROUP) 
			return IDX_NEWSNEW; 
		else
			return IDX_NEWSGROUP; 
	}
}

int CNewsgroupsOutliner::TranslateIconFolder (void * pData)
{
    ASSERT(pData);

    MSG_GroupNameLine *pGroup = (MSG_GroupNameLine*)pData;
	
	if (pGroup->flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
	{
		if (pGroup->flags & MSG_GROUPNAME_FLAG_ELIDED) 
            return(OUTLINER_CLOSEDFOLDER);  
		else 
            return(OUTLINER_OPENFOLDER);
	}
    return (OUTLINER_ITEM);
}

BOOL CNewsgroupsOutliner::ColumnCommand(int iColumn, int iLine)
{
	if (iColumn == ID_COLNEWS_SUBSCRIBE)
	{
		MSG_ViewIndex indices;
		indices = (MSG_ViewIndex)iLine;
		MSG_Command(GetPane(), MSG_ToggleSubscribed, &indices, 1);
	}
    return FALSE;
}


void* CNewsgroupsOutliner::AcquireLineData(int line)
{
	MSG_Pane *pPane = GetPane();
	if (pPane && m_pPage)
	{
		if (MSG_SubscribeGetMode(pPane) != m_pPage->GetMode())
			return NULL;
	}
	else
		return NULL;
	
	delete [] m_pAncestor;
	m_pAncestor = NULL;

    m_pszExtraText[ 0 ] = '\0';
	if ( line >= m_iTotalLines)
		return NULL;

	if ( !MSG_GetGroupNameLineByIndex(m_pPane, line, 1, &m_GroupLine ))
        return NULL;

	return &m_GroupLine;
}


void CNewsgroupsOutliner::GetTreeInfo( int iLine, uint32 * pFlags, int* pDepth, 
									   OutlinerAncestorInfo** pAncestor)
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

	MSG_GroupNameLine groupLine;
	MSG_GetGroupNameLineByIndex(m_pPane, (MSG_ViewIndex) iLine, 1, &groupLine );

	if ( pAncestor ) {
		m_pAncestor = new OutlinerAncestorInfo[groupLine.level + 1];

		int i = groupLine.level;
		int idx = iLine + 1;
		while ( i > 0 ) {
			if ( idx < m_iTotalLines ) {
				MSG_GroupNameLine groupLine;
				MSG_GetGroupNameLineByIndex( m_pPane, idx, 1, &groupLine );
				if ( groupLine.level == i ) {
					m_pAncestor[i].has_prev = TRUE;
					m_pAncestor[i].has_next = TRUE;
					i--;
					idx++;
				} else if ( groupLine.level < i ) {
					m_pAncestor[i].has_prev = FALSE;
					m_pAncestor[i].has_next = FALSE;
					i--;
				} else {
					idx++;
				}
			} else {
				m_pAncestor[i].has_prev = FALSE;
				m_pAncestor[i].has_next = FALSE;
				i--;
			}
		}
		m_pAncestor[i].has_prev = FALSE;
		m_pAncestor[i].has_next = FALSE;

        *pAncestor = m_pAncestor;
	}

    if ( pFlags ) *pFlags = groupLine.flags;
    if ( pDepth ) *pDepth = groupLine.level;// do not -1 
}


void CNewsgroupsOutliner::ReleaseLineData(void *)
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;
}


LPCTSTR CNewsgroupsOutliner::GetColumnText(UINT iColumn, void* pLineData)
{
	MSG_GroupNameLine* pGroup = (MSG_GroupNameLine*)pLineData;

	memset(m_pszExtraText, '\0',  256);
	switch (iColumn) 
	{
		case ID_COLNEWS_NAME:
			strncpy(m_pszExtraText, pGroup->name, strlen(pGroup->name));
			break;
		case ID_COLNEWS_POSTINGS:
			if (pGroup->total >= 0) {
				sprintf(m_pszExtraText, "%ld", pGroup->total);
			}
			break;
		default:
			break;
	} 
	return m_pszExtraText;
}																	

void CNewsgroupsOutliner::OnSelChanged()
{
	if (!GetPage())
		return;
	if (GetPage()->m_bFromTyping)
	{
		GetPage()->m_bFromTyping = FALSE;
		return;
	}
	MSG_GroupNameLine group;

	MSG_ViewIndex *indices;
	int count;

	GetSelection( indices, count );
	if ( count == 1 && MSG_GetGroupNameLineByIndex(m_pPane, indices[0], 1, &group))
		GetPage()->DoSelChanged(&group);
}

void CNewsgroupsOutliner::OnSelDblClk()
{
	MSG_ViewIndex *indices;
	int count;
	MSG_GroupNameLine group;

	GetSelection( indices, count );
	if ( count == 1 && MSG_GetGroupNameLineByIndex(m_pPane, indices[0], 1, &group))
	{
		if (group.flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
			ToggleExpansion(indices[0]);
		else
			MSG_Command(GetPane(), MSG_ToggleSubscribed, &indices[0], 1);
	}
}

int CNewsgroupsOutliner::ToggleExpansion(int iLine)
{
	int count;
	MSG_ViewIndex *indices;
	MSG_GroupNameLine group;

	int nResult = CMailNewsOutliner::ToggleExpansion(iLine);

	GetSelection(indices, count);

	if (indices[0] == iLine && 
		MSG_GetGroupNameLineByIndex(m_pPane, iLine, 1, &group) &&
		GetPage()->GetMode() == MSG_SubscribeAll)
	{
		if (group.flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
		{
			if (group.flags & MSG_GROUPNAME_FLAG_ELIDED) 
				GetPage()->GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(TRUE);  
			else 
				GetPage()->GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
		else
		{
			GetPage()->GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
	}
	return nResult;
}

// for select the first item in the outliner,
// so user can tab through
BOOL CNewsgroupsOutliner::SelectInitialItem()
{
	int count;
	MSG_ViewIndex *indices;
	GetSelection( indices, count );

	if (GetPage() && GetPage()->m_bFromTyping)
		return TRUE;

	if (GetTotalLines() && !count )
	{
		SelectItem(0);
		InvalidateLine(0); 
		return TRUE;
	}
	if (count)
		return TRUE;
	return FALSE;
}

/////////////////////////////////////////////////////////////////////////////
// CSubscribePropertyPage

CSubscribePropertyPage::CSubscribePropertyPage(CWnd *pParent,
	MWContext *pContext, MSG_SubscribeMode nMode, UINT nID)
	: CNetscapePropertyPage(nID)
{
	m_pParent = (CSubscribePropertySheet*)pParent;
    m_bActivated = FALSE;
	m_bSelChanged = FALSE;
	m_bNotifyAll = FALSE;
	m_bInitDialog = FALSE;
	m_bFromTyping = FALSE;
	m_bDoShowWindow = TRUE;
	m_nMode = nMode;
	m_uTimer = 0;

	m_hNewsHost = NULL;
	m_pOutliner = NULL;

	m_bProcessGetDeletion = FALSE;
	m_bListChangeStarting = FALSE;
}

CSubscribePropertyPage::~CSubscribePropertyPage()
{
	if (m_hNewsHost)
		delete [] m_hNewsHost;
	m_hNewsHost = NULL;
}

MWContext* CSubscribePropertyPage::GetContext()
{
	CSubscribePropertySheet* pSheet = (CSubscribePropertySheet*)GetParent();
	CSubscribeCX*  pCX = pSheet->GetSubscribeContext();
	if (pCX)
		return pCX->GetContext();
	else
		return NULL;
}

CSubscribeCX * CSubscribePropertyPage::GetSubscribeContext() 
	{ return ((CSubscribePropertySheet*)GetParent())->GetSubscribeContext(); }

MSG_Pane * CSubscribePropertyPage::GetPane() 
	{ return ((CSubscribePropertySheet*)GetParent())->GetSubscribePane(); }

CSubscribeList*  CSubscribePropertyPage::GetList()  		
	{ return ((CSubscribePropertySheet*)GetParent())->GetSubscribeList(); }

CSubscribeList**  CSubscribePropertyPage::GetListHandle()  		
	{ return ((CSubscribePropertySheet*)GetParent())->GetSubscribeHandle(); }

void CSubscribePropertyPage::SetSubscribeContext(CSubscribeCX *pCX)  
	{ ((CSubscribePropertySheet*)GetParent())->SetSubscribeContext(pCX); }

void CSubscribePropertyPage::SetPane(MSG_Pane *pPane)  
	{ ((CSubscribePropertySheet*)GetParent())->SetSubscribePane(pPane); }

void CSubscribePropertyPage::SetList(CSubscribeList* pList) 
	{ ((CSubscribePropertySheet*)GetParent())->SetSubscribeList(pList); }

BOOL CSubscribePropertyPage::IsOutlinerHasFocus()
{
	if (GetFocus() == m_pOutliner)
		return TRUE;
	else
		return FALSE;
}


static void
DoFetchGroups(MSG_Pane* pane, void* closure)
{
    CSubscribePropertySheet* tmp = (CSubscribePropertySheet*) closure;
    tmp->GetAllGroupPage()->OnGetDeletions();
}

static void
DoEnableAllControls(MSG_Pane* pane, void* closure)
{
    CSubscribePropertySheet* tmp = (CSubscribePropertySheet*) closure;
	if (tmp->GetAllGroupPage()->m_bProcessGetDeletion)
	{
		tmp->GetAllGroupPage()->m_bProcessGetDeletion = FALSE;
		tmp->GetAllGroupPage()->EnableAllControls(TRUE);
	}

	CNewsgroupsOutliner* pOutliner = tmp->GetAllGroupPage()->GetOutliner();
	if (pOutliner)
		pOutliner->SelectInitialItem();
}

static MSG_SubscribeCallbacks CallbackStruct = {
    DoFetchGroups,
    DoEnableAllControls			
};


BOOL CSubscribePropertyPage::OnInitDialog()
{
	BOOL ret = CNetscapePropertyPage::OnInitDialog();

	m_ServerCombo.SubclassDlgItem( IDC_COMBO_SERVER, this );

	m_bInitDialog = TRUE;

  	CRect winRect, ctlRect;
	GetDlgItem(IDC_NEWSGROUPS)->ShowWindow( SW_HIDE );
	GetDlgItem(IDC_NEWSGROUPS)->GetWindowRect(ctlRect);
	GetWindowRect(winRect);
	ctlRect.OffsetRect(-winRect.left, -winRect.top);
	ctlRect.InflateRect(-1, -1);

#ifdef _WIN32
	m_OutlinerParent.CreateEx ( WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"), 
							WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_TABSTOP,
							ctlRect.left, ctlRect.top, 
							ctlRect.right - ctlRect.left, ctlRect.bottom - ctlRect.top,
							this->m_hWnd, NULL);
#else
	m_OutlinerParent.Create( NULL, _T("NSOutlinerParent"), 
							 WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_TABSTOP,
							 ctlRect, this, NULL);
#endif

	m_OutlinerParent.SetPropertySheet(m_pParent);
	m_OutlinerParent.CreateColumns();
	m_pOutliner = (CNewsgroupsOutliner *) m_OutlinerParent.m_pOutliner;
	m_pOutliner->SetPage(this);

	if (m_nMode != MSG_SubscribeNew)
	{
	#ifdef _WIN32
		((CEdit*)GetDlgItem(IDC_EDIT_NEWSGROUP))->SetLimitText(MSG_MAXGROUPNAMELENGTH - 1);
	#else
		((CEdit*)GetDlgItem(IDC_EDIT_NEWSGROUP))->LimitText(MSG_MAXGROUPNAMELENGTH - 1);
	#endif
	}
	if (m_pOutliner)
		m_pOutliner->SelectInitialItem();
	return ret;
}

void CSubscribePropertyPage::OnShowWindow(BOOL bShow, UINT nStatus)
{
	if (bShow)
	{   
		if (m_uTimer == 0)
			m_uTimer = SetTimer(SUBSCRIBE_VISIBLE_TIMER, 100, NULL);
		if (m_uTimer)
			m_bDoShowWindow = TRUE;
		else   
		{
			if (!InitSubscribePage())
				m_pParent->EndDialog(IDCANCEL);
		}
	}
}

BOOL CSubscribePropertyPage::InitSubscribePage()
{
	BOOL result = TRUE;

	if (m_bInitDialog)
	{
		m_bInitDialog = FALSE;
		result = CreateSubscribePage();
	}
	if (result && GetPane() &&
		m_nMode != MSG_SubscribeGetMode(GetPane()))
	{
		MSG_SubscribeSetMode(GetPane(), m_nMode); 
		if (GetList())
			GetList()->SetSubscribePage(this);
		m_pOutliner->SetPane(GetPane());
	}
	return result;
}

BOOL CSubscribePropertyPage::CreateSubscribePage()
{
	MSG_Master * pMaster = WFE_MSGGetMaster();

	SetNewsHosts(pMaster);

	//create CSubscribePane for subscribe newsgroup
	MSG_Pane* pPane = GetPane();
	if (pPane == NULL)
	{
		CSubscribeCX* pCX = new CSubscribeCX(m_pParent);

		pCX->GetContext()->fancyFTP = TRUE;
		pCX->GetContext()->fancyNews = TRUE;
		pCX->GetContext()->intrupt = FALSE;
		pCX->GetContext()->reSize = FALSE;
		pCX->GetContext()->type = MWContextNews;
		SetSubscribeContext(pCX);

		MSG_Host *pCurrentHost = ((CSubscribePropertySheet*)GetParent())->GetHost();
 		pPane = MSG_CreateSubscribePaneForHost(pCX->GetContext(), pMaster, pCurrentHost);

		if (!pPane)
			return FALSE;

		((CSubscribePropertySheet*)GetParent())->SetSubscribePane(pPane);
		if (m_pOutliner)
			m_pOutliner->SetPane(pPane);

		//use  CSubscribeList to hook up with the backend
		CSubscribeList *pInstance = new CSubscribeList(this);
		SetList(pInstance);
		CSubscribeList** hList = GetListHandle();
		pInstance->QueryInterface(IID_IMsgList, (LPVOID *)hList);
		MSG_SetFEData((MSG_Pane*)pPane, (void *)pInstance);

		if (GetList())
			GetList()->SetSubscribePage(this);

		MSG_SubscribeSetCallbacks(pPane, &CallbackStruct, GetParent());

	}

	return TRUE;

}

void CSubscribePropertyPage::SetNewsHosts(MSG_Master* pMaster)
{
	int nDefaultHost = 0;
	MSG_Host *pNewsHost = NULL;

	int32 nTotal =  MSG_GetSubscribingHosts(pMaster, NULL, 0);
	if (nTotal)
	{
		m_hNewsHost = new MSG_Host* [nTotal];
		MSG_GetSubscribingHosts(pMaster, m_hNewsHost, nTotal);
		for (int i = 0; i < nTotal; i++)
		{
			int addedIndex = GetServerCombo()->AddString(MSG_GetHostUIName(m_hNewsHost[i]));
			if (addedIndex != CB_ERR)
			{
				GetServerCombo()->SetItemDataPtr(addedIndex, m_hNewsHost[i]);
			}
		}
		MSG_Host *pCurrentHost = ((CSubscribePropertySheet*)GetParent())->GetHost();
		if (pCurrentHost)
		{
			for (i = 0; i < nTotal; i++)
			{
				pNewsHost = (MSG_Host *) GetServerCombo()->GetItemDataPtr(i);
				if (pNewsHost == pCurrentHost)
				{
					nDefaultHost = i;
					break;
				}
			}
		}
		else
		{
			MSG_Host* pDefhost = MSG_GetMSGHostFromNewsHost(MSG_GetDefaultNewsHost(pMaster));
			for (i = 0; i < nTotal; i++)
			{
				pNewsHost = (MSG_Host *) GetServerCombo()->GetItemDataPtr(i);
				if 	(pDefhost == pNewsHost)
				{
					nDefaultHost = i;
					((CSubscribePropertySheet*)GetParent())->SetHost(pNewsHost);
					break;
				}
			}
		}
		GetServerCombo()->SetCurSel(i);
		if (pNewsHost && MSG_IsIMAPHost(pNewsHost))
			m_pParent->EnableNonImapPages(FALSE);
	}
}

BOOL CSubscribePropertyPage::OnSetActive()
{
    if(!CNetscapePropertyPage::OnSetActive())
        return(FALSE);

    if(!m_bActivated)
		m_bActivated = TRUE;

	if (m_pParent)  
		m_pParent->SetStatusText("");

	if (GetOutliner())
	{
		GetOutliner()->SetColumnSize(ID_COLNEWS_NAME, m_pParent->nameWidth);
		GetOutliner()->SetColumnSize(ID_COLNEWS_SUBSCRIBE, m_pParent->subscribeWidth);
		GetOutliner()->SetColumnSize(ID_COLNEWS_POSTINGS, m_pParent->postWidth);
		GetOutliner()->SetColumnPos(ID_COLNEWS_NAME, m_pParent->namePos);
		GetOutliner()->SetColumnPos(ID_COLNEWS_SUBSCRIBE, m_pParent->subscribePos);
		GetOutliner()->SetColumnPos(ID_COLNEWS_POSTINGS, m_pParent->postPos);
	}

	ClearNewsgroupSelection();
	//set news host
	int nSelection = GetServerCombo()->GetCurSel();
	MSG_Host *pSelHost = (MSG_Host *)GetServerCombo()->GetItemDataPtr(nSelection);
	MSG_Host *pCurrentHost = ((CSubscribePropertySheet*)GetParent())->GetHost();
	if (pSelHost !=  pCurrentHost)	//not the current selected server
	{
		int nNewIndex = 0;
		int nTotal = GetServerCombo()->GetCount();
		for (int i = 0; i < nTotal; i++)
		{
			MSG_Host *pNewsHost = (MSG_Host *)GetServerCombo()->GetItemDataPtr(i);
			if (pNewsHost == pCurrentHost)
			{
				nNewIndex = i;
				break;
			}
		}
		// the checking id for if can't fiind a match and nSelection != nNewIndex
		if (nNewIndex != nSelection)  
			GetServerCombo()->SetCurSel(nNewIndex);
	}
 
	if (GetOutliner())
	{
		if (GetOutliner()->SelectInitialItem())
		{
			int count;
			MSG_ViewIndex *indices;
			MSG_GroupNameLine group;

			GetOutliner()->GetSelection( indices, count );
			if (MSG_GetGroupNameLineByIndex(GetPane(), indices[0], 1, &group))
				CheckSubscribeButton(&group);
		}
		if (m_nMode == MSG_SubscribeNew)
		{
			if (GetOutliner()->GetTotalLines())
				GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(TRUE);
			else
				GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(FALSE);
		}
	}

	return(TRUE);
}

BOOL CSubscribePropertyPage::OnKillActive()
{
	m_pParent->nameWidth = GetOutliner()->GetColumnSize(ID_COLNEWS_NAME);
	m_pParent->subscribeWidth = GetOutliner()->GetColumnSize(ID_COLNEWS_SUBSCRIBE);
	m_pParent->postWidth = GetOutliner()->GetColumnSize(ID_COLNEWS_POSTINGS);
	m_pParent->namePos = GetOutliner()->GetColumnPos(ID_COLNEWS_NAME);
	m_pParent->subscribePos = GetOutliner()->GetColumnPos(ID_COLNEWS_SUBSCRIBE);
	m_pParent->postPos = GetOutliner()->GetColumnPos(ID_COLNEWS_POSTINGS);

	ShowWindow(SW_HIDE); // Hide window to prevent flash list

	return CNetscapePropertyPage::OnKillActive();
}

void CSubscribePropertyPage::DoSelChanged(MSG_GroupNameLine* pGroup)
{
	if (m_nMode == MSG_SubscribeAll)
	{
		
		if (!(pGroup->flags & MSG_GROUPNAME_FLAG_HASCHILDREN))
		{
			m_bSelChanged = TRUE;
			GetDlgItem(IDC_EDIT_NEWSGROUP)->SetWindowText(pGroup->name);
		}

		if (m_bProcessGetDeletion)
			return;

		if (pGroup->flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
		{
			if (pGroup->flags & MSG_GROUPNAME_FLAG_ELIDED) 
				GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(TRUE);  
			else 
				GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
		else
		{
			GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
	}
	CheckSubscribeButton(pGroup);
}

void CSubscribePropertyPage::CheckSubscribeButton(MSG_GroupNameLine* pGroup)
{
	if (!pGroup)
		return;
	
	if (pGroup->flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
	{
		GetDlgItem(IDC_SUBSCRIBE)->EnableWindow(FALSE);  
		GetDlgItem(IDC_UNSUBSCRIBE)->EnableWindow(FALSE);  
	}
	else
	{	
		GetDlgItem(IDC_SUBSCRIBE)->EnableWindow(TRUE);
		GetDlgItem(IDC_UNSUBSCRIBE)->EnableWindow(TRUE);  
	}
}

void CSubscribePropertyPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CSubscribePropertyPage::CleanupOnClose()
{
	CSubscribeList* pList = GetList();
	if (pList)
	{
		pList->Release();
		SetList(NULL);
	}

	CSubscribeCX* pCX = GetSubscribeContext();
	if (pCX)
	{
		if(!pCX->IsDestroyed())  
			pCX->DestroyContext();
		SetSubscribeContext(NULL);
	}
	MSG_Pane * pPane = GetPane();
	if (pPane)
	{
		MSG_DestroyPane(pPane);	
		SetPane(NULL);
	}

}

void CSubscribePropertyPage::OnOK() 
{
    if (!m_bActivated)	// this is to fix the crash below when m_pOutliner 
        return;			// is NULL.

	if (m_bProcessGetDeletion && GetSubscribeContext())
	{
		if (XP_IsContextBusy(GetContext()))
			XP_InterruptContext(GetContext());
	}
	CPropertyPage::OnOK();
	CleanupOnClose();
}

void CSubscribePropertyPage::OnCancel() 
{
	if (m_bProcessGetDeletion && GetSubscribeContext())
	{
		if (XP_IsContextBusy(GetContext()))
			XP_InterruptContext(GetContext());
	}
	if (GetPane())
		MSG_SubscribeCancel(GetPane());

	CNetscapePropertyPage::OnCancel();
	CleanupOnClose();
}

void CSubscribePropertyPage::OnAddServer()
{
	CServerTypeDialog serverTypeDialog(this->GetParent());

	if (IDOK == serverTypeDialog.DoModal())
	{
		MSG_Host *pNewHost = serverTypeDialog.GetNewHost();
					 
		if (pNewHost)
		{
			int nIndex = GetServerCombo()->AddString(MSG_GetHostUIName(pNewHost));

			((CSubscribePropertySheet*)GetParent())->AddServer(pNewHost);
			if (nIndex != CB_ERR)
			{
				m_bFromTyping = FALSE;
				GetServerCombo()->SetItemDataPtr(nIndex, pNewHost);
				GetServerCombo()->SetCurSel(nIndex);
				OnChangeServer();
			}
		}
	}
}

void CSubscribePropertyPage::OnSubscribeNewsgroup()
{
	int count;
	MSG_ViewIndex *indices;
	MSG_GroupNameLine group;

	m_pOutliner->GetSelection( indices, count );
	for (int i = 0; i < count; i++)
	{
		MSG_GetGroupNameLineByIndex(GetPane(), indices[i], 1, &group);
		if (!(group.flags & MSG_GROUPNAME_FLAG_SUBSCRIBED))
			MSG_Command(GetPane(), MSG_ToggleSubscribed, &indices[i], 1);
	}
	//Get updated info
	MSG_GetGroupNameLineByIndex(GetPane(), indices[0], 1, &group);
	CheckSubscribeButton(&group);
}

void CSubscribePropertyPage::OnUnsubscribeNewsgroup()
{
	int count;
	MSG_ViewIndex *indices;
	MSG_GroupNameLine group;

	m_pOutliner->GetSelection( indices, count );
	for (int i = 0; i < count; i++)
	{
		MSG_GetGroupNameLineByIndex(GetPane(), indices[i], 1, &group);
		if (group.flags & MSG_GROUPNAME_FLAG_SUBSCRIBED)
			MSG_Command(GetPane(), MSG_ToggleSubscribed, &indices[i], 1);
	}
	//Get updated info
	MSG_GetGroupNameLineByIndex(GetPane(), indices[0], 1, &group);
	CheckSubscribeButton(&group);
}

void CSubscribePropertyPage::OnChangeServer()
{
	CSubscribePropertySheet* pParent = (CSubscribePropertySheet*)GetParent();
	MSG_Host *pCurrentHost = pParent->GetHost();
	int nIndex = GetServerCombo()->GetCurSel();
	MSG_Host *pHost = (MSG_Host *)GetServerCombo()->GetItemDataPtr(nIndex);
	if (pHost == pCurrentHost)
		return;

	DoStopListChange();
	ClearNewsgroupSelection();
	if (m_nMode != MSG_SubscribeNew && GetDlgItem(IDC_EDIT_NEWSGROUP))
	{
		GetDlgItem(IDC_EDIT_NEWSGROUP)->SetWindowText("");
		//SetWindowText() caused call to OnChangeNewsgroup() and set 
		//m_bFromTyping == TRUE,  reset it back to FALSE 
		m_bFromTyping = FALSE;	  
	}
	GetOutliner()->SetTotalLines(0);	// fix for bug# 38007
	pParent->SetHost(pHost);
	MSG_SubscribeSetHost(GetPane(), pHost);
	if (MSG_IsIMAPHost(pHost))
		m_pParent->EnableNonImapPages(FALSE);
	else
		m_pParent->EnableNonImapPages(TRUE);
}

void CSubscribePropertyPage::EnableAllControls(BOOL bEnable)
{
    CWnd *wnd = NULL;
    if (wnd = GetDlgItem(IDC_EDIT_NEWSGROUP))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_SUBSCRIBE))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_UNSUBSCRIBE))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_EXPAND_ALL))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_COLLAPSE_ALL))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_GET_DELETE))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_ADD_SERVER))
      wnd->EnableWindow(bEnable);
    if (wnd = GetDlgItem(IDC_COMBO_SERVER))
      wnd->EnableWindow(bEnable);
    if (wnd = GetParent()->GetDlgItem(IDOK))
      wnd->EnableWindow(bEnable);
	if (bEnable == TRUE)
		m_pParent->StopAnimation();

}

void CSubscribePropertyPage::DoStopListChange()
{
	if (m_bListChangeStarting)
	{
		if (GetSubscribeContext() && XP_IsContextBusy(GetContext()))
			XP_InterruptContext(GetContext());
		m_bListChangeStarting = FALSE;
	}
}

void CSubscribePropertyPage::ClearNewsgroupSelection()
{
	if (m_pOutliner && m_pOutliner->IsWindowVisible())
	{
		if (m_pOutliner->GetTotalLines())
		{
			int count;
			MSG_ViewIndex *indices;
			m_pOutliner->GetSelection(indices, count);

			if (count)
				m_pOutliner->SelectItem(-1);
		}
	}
}

void CSubscribePropertyPage::ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
	MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
	if ( pane == GetPane() ) 
	{
		m_bListChangeStarting = TRUE;
		
		if (m_pOutliner) 
			m_pOutliner->MysticStuffStarting(asynchronous, notify, where, num );
	}
}

void CSubscribePropertyPage::OnTimer(UINT nIDEvent)
{
	if (m_uTimer == nIDEvent)
	{
		if (m_bNotifyAll && m_pOutliner->IsWindowVisible())
		{
			m_pOutliner->MysticStuffFinishing(m_bAsynchronous, MSG_NotifyAll, 0, 0);
			m_pOutliner->SetTotalLines(CASTINT(MSG_GetNumLines(GetPane())));
			m_bNotifyAll = FALSE;
			KillTimer(m_uTimer);
			m_uTimer = 0;
		}
		if (IsWindowVisible() && m_bDoShowWindow)
		{
			BOOL result = InitSubscribePage();
			m_bDoShowWindow = FALSE;
			KillTimer(m_uTimer);
			m_uTimer = 0;
			if (!result)
				m_pParent->EndDialog(IDCANCEL);

		}
	}
}

void CSubscribePropertyPage::ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
	MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
	if ( pane == GetPane() )
	{
		m_bListChangeStarting = FALSE;
		if (m_pOutliner)
		{
			//check if there's a MSG_NotifyAll and the outliner is not visible,
			//when it is visible, do MSG_NotifyAll at that time
			if (notify == MSG_NotifyAll && !m_pOutliner->IsWindowVisible())
			{
				m_uTimer = SetTimer(SUBSCRIBE_VISIBLE_TIMER, 100, NULL);
				if (m_uTimer)
				{
					m_bNotifyAll = TRUE;
					m_bAsynchronous = asynchronous;
					return;
				}
			}
			if (m_bNotifyAll && m_pOutliner->IsWindowVisible() &&
				notify != MSG_NotifyAll)
			{
				if (notify != MSG_NotifyAll)
					m_pOutliner->MysticStuffFinishing(asynchronous, MSG_NotifyAll, 0, 0);
				m_bNotifyAll = FALSE;
			}
			m_pOutliner->MysticStuffFinishing(asynchronous, notify, where, num);
			m_pOutliner->SetTotalLines(CASTINT(MSG_GetNumLines(GetPane())));
		}
	}
}

BEGIN_MESSAGE_MAP(CSubscribePropertyPage, CNetscapePropertyPage)
	ON_WM_TIMER()
	ON_WM_SHOWWINDOW()
    ON_BN_CLICKED(IDC_ADD_SERVER, OnAddServer)
    ON_BN_CLICKED(IDC_SUBSCRIBE, OnSubscribeNewsgroup)
    ON_BN_CLICKED(IDC_UNSUBSCRIBE, OnUnsubscribeNewsgroup)
    ON_CBN_SELCHANGE(IDC_COMBO_SERVER, OnChangeServer)
	ON_BN_CLICKED(IDOK, OnOK)
	ON_BN_CLICKED(IDCANCEL, OnCancel)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAllNewsgroupsPage

CAllNewsgroupsPage::CAllNewsgroupsPage(CWnd *pParent,
	MWContext *pContext, MSG_SubscribeMode nMode)
	: CSubscribePropertyPage(pParent, pContext, nMode, IDD)
{
}

BOOL CAllNewsgroupsPage::OnInitDialog()
{
	BOOL ret = CSubscribePropertyPage::OnInitDialog();

	return ret;
}

BOOL CAllNewsgroupsPage::OnKillActive()
{
	OnStop();

    return CSubscribePropertyPage::OnKillActive();
}

void CAllNewsgroupsPage::OnChangeNewsgroup()
{
	if (m_bSelChanged)	
	{	//change from user click outliner, change the selection
		m_bSelChanged = FALSE;
		return;
	}
	m_bFromTyping = TRUE;

	CString group;
	GetDlgItem(IDC_EDIT_NEWSGROUP)->GetWindowText(group);
	MSG_ViewIndex index = MSG_SubscribeFindFirst(GetPane(), 
												LPCTSTR(group));

	if (index != MSG_VIEWINDEXNONE )
	{
		MSG_GroupNameLine newsGroup;

		m_pOutliner->SelectItem(CASTINT(index));
		m_pOutliner->ScrollIntoView(CASTINT(index));
		int len = group.GetLength();
 		((CEdit*)GetDlgItem(IDC_EDIT_NEWSGROUP))->SetSel(len, len);
		if (MSG_GetGroupNameLineByIndex(GetPane(), index, 1, &newsGroup))
			CheckSubscribeButton(&newsGroup);
	} 
}

void CAllNewsgroupsPage::OnExpandAll()
{
	DoStopListChange();
	MSG_ViewIndex *indices;
	int count;
	m_pOutliner->GetSelection( indices, count );
	MSG_Command(GetPane(), MSG_ExpandAll, indices, count);
	GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
}

void CAllNewsgroupsPage::OnCollapseAll()
{
	int count;
	MSG_ViewIndex *indices;
	MSG_GroupNameLine group;

	DoStopListChange();
	MSG_Command(GetPane(), MSG_CollapseAll, NULL, 0);

	m_pOutliner->GetSelection(indices, count);
	if (MSG_GetGroupNameLineByIndex(GetPane(), indices[0], 1, &group))
	{
		if (group.flags & MSG_GROUPNAME_FLAG_HASCHILDREN)
		{
			if (group.flags & MSG_GROUPNAME_FLAG_ELIDED) 
				GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(TRUE);  
			else 
				GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
		else
		{
			GetDlgItem(IDC_EXPAND_ALL)->EnableWindow(FALSE);  
		}
	}
}

void CAllNewsgroupsPage::OnGetDeletions()
{
	if (IsWindowVisible())
		ClearNewsgroupSelection();
	m_bProcessGetDeletion = TRUE;
	EnableAllControls(FALSE);
	m_pParent->StartAnimation();
	MSG_Command(GetPane(), MSG_FetchGroupList, NULL, 0);
}

void CAllNewsgroupsPage::OnStop()
{
	m_pParent->StopAnimation();
	if (GetSubscribeContext() && XP_IsContextBusy(GetContext()))
		XP_InterruptContext(GetContext());
	if (m_bListChangeStarting)
		m_bListChangeStarting = FALSE;
	if (m_bProcessGetDeletion)
	{
		m_bProcessGetDeletion = FALSE;
		EnableAllControls(TRUE);
	}
	GetOutliner()->SelectInitialItem();
}

void CAllNewsgroupsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAllNewsgroupsPage, CSubscribePropertyPage)
    ON_EN_CHANGE(IDC_EDIT_NEWSGROUP, OnChangeNewsgroup)
    ON_BN_CLICKED(IDC_EXPAND_ALL, OnExpandAll)
    ON_BN_CLICKED(IDC_COLLAPSE_ALL, OnCollapseAll)
    ON_BN_CLICKED(IDC_GET_DELETE, OnGetDeletions)
    ON_BN_CLICKED(IDC_STOP, OnStop)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSearchNewsgroupPage

CSearchNewsgroupPage::CSearchNewsgroupPage(CWnd *pParent,
	MWContext *pContext, MSG_SubscribeMode nMode)
	: CSubscribePropertyPage(pParent, pContext, nMode, IDD)
{
}

BOOL CSearchNewsgroupPage::OnInitDialog()
{
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);

	BOOL ret = CSubscribePropertyPage::OnInitDialog();
	return ret;
}

void CSearchNewsgroupPage::OnSearchNow()
{
	CString group;

	ClearNewsgroupSelection();
	GetDlgItem(IDC_EDIT_NEWSGROUP)->GetWindowText(group);
	GetDlgItem(IDC_STOP)->EnableWindow(TRUE);
	MSG_SubscribeFindAll(GetPane(), LPCTSTR(group));
	if (m_pOutliner->GetTotalLines())
		m_pOutliner->SelectItem(0);
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
}

void CSearchNewsgroupPage::OnStop()
{
	m_pParent->StopAnimation();
	if (GetSubscribeContext() && XP_IsContextBusy(GetContext()))
		XP_InterruptContext(GetContext());
	if (m_bListChangeStarting)
		m_bListChangeStarting = FALSE;
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
}

void CSearchNewsgroupPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSearchNewsgroupPage, CSubscribePropertyPage)
    ON_BN_CLICKED(IDC_SEARCH_NOW, OnSearchNow)
    ON_BN_CLICKED(IDC_STOP, OnStop)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CNewNewsgroupsPage

CNewNewsgroupsPage::CNewNewsgroupsPage(CWnd *pParent,
	MWContext *pContext, MSG_SubscribeMode nMode)
	: CSubscribePropertyPage(pParent, pContext, nMode, IDD)
{
	m_bGetNew = FALSE;
}

BOOL CNewNewsgroupsPage::OnInitDialog()
{
	GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
	return  CSubscribePropertyPage::OnInitDialog();
}

BOOL CNewNewsgroupsPage::OnKillActive()
{
	OnStop();

    return CSubscribePropertyPage::OnKillActive();
}

void CNewNewsgroupsPage::OnGetNew()
{
	m_pParent->StartAnimation();
	MSG_Command(GetPane(), MSG_CheckForNew, NULL, 0);
	GetDlgItem(IDC_GET_NEW)->EnableWindow(FALSE);
	GetDlgItem(IDC_STOP)->EnableWindow(TRUE);
	m_bGetNew = TRUE;
}

void CNewNewsgroupsPage::OnClearNew()
{
 	ClearNewsgroupSelection();
	GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(FALSE);
	MSG_Command(GetPane(), MSG_ClearNew, NULL, 0);
}

void CNewNewsgroupsPage::OnStop()
{
	if (GetSubscribeContext() && XP_IsContextBusy(GetContext()))
		XP_InterruptContext(GetContext());
	if (m_bListChangeStarting)
		m_bListChangeStarting = FALSE;

	if (m_bGetNew)
	{
		m_bGetNew = FALSE;
		m_pParent->StopAnimation();
		GetDlgItem(IDC_GET_NEW)->EnableWindow(TRUE);
		if (GetOutliner()->GetTotalLines())
			GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(TRUE);
		else
			GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(FALSE);
		GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
	}
}

void CNewNewsgroupsPage::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CNewNewsgroupsPage, CSubscribePropertyPage)
    ON_BN_CLICKED(IDC_GET_NEW, OnGetNew)
    ON_BN_CLICKED(IDC_CLEAR_NEW, OnClearNew)
    ON_BN_CLICKED(IDC_STOP, OnStop)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CSubscibePropertySheet		   
CSubscribePropertySheet::CSubscribePropertySheet
(CWnd *pParent, MWContext *pContext, const char* pName)
	: CNetscapePropertySheet(pName, pParent, 0, pContext)
{
	m_pCX = NULL;
	m_pSubscribePane = NULL;
	m_pSubscribeList = NULL;
	m_pCurrentHost = NULL;
	m_pNewPage = NULL;

	m_bCommitingStart = FALSE;

	nameWidth = -1;
	subscribeWidth = -1;
	postWidth = -1;
	namePos = -1;
	subscribePos = -1;
	postPos = -1;

	int32 lPrefInt;
	PREF_GetIntPref("news.subscribe.name_width",&lPrefInt);
	nameWidth = CASTINT(lPrefInt);
	PREF_GetIntPref("news.subscribe.join_width",&lPrefInt);
	subscribeWidth = CASTINT(lPrefInt);
	PREF_GetIntPref("news.subscribe.post_width",&lPrefInt);
	postWidth = CASTINT(lPrefInt);
	PREF_GetIntPref("news.subscribe.name_pos",&lPrefInt);
	namePos = CASTINT(lPrefInt);
	PREF_GetIntPref("news.subscribe.join_pos",&lPrefInt);
	subscribePos = CASTINT(lPrefInt);
	PREF_GetIntPref("news.subscribe.post_pos",&lPrefInt);
	postPos = CASTINT(lPrefInt);

	m_pAllGroupPage = new CAllNewsgroupsPage(this, pContext);
	m_pSearchGroupPage = new CSearchNewsgroupPage(this, pContext);
	m_pNewGroupPage = new CNewNewsgroupsPage(this, pContext);

    AddPage(m_pAllGroupPage);
    AddPage(m_pSearchGroupPage);
    AddPage(m_pNewGroupPage);
}

CSubscribePropertySheet::~CSubscribePropertySheet()
{
	PREF_SetIntPref("news.subscribe.name_width", (int32)nameWidth);
	PREF_SetIntPref("news.subscribe.join_width", (int32)subscribeWidth);
	PREF_SetIntPref("news.subscribe.post_width", (int32)postWidth);
	PREF_SetIntPref("news.subscribe.name_pos", (int32)namePos);
	PREF_SetIntPref("news.subscribe.join_pos", (int32)subscribePos);
	PREF_SetIntPref("news.subscribe.post_pos", (int32)postPos);

	if (m_pAllGroupPage)
		delete m_pAllGroupPage;
	if (m_pSearchGroupPage)
		delete m_pSearchGroupPage;
	if (m_pNewGroupPage)
		delete m_pNewGroupPage;   
}

BOOL CSubscribePropertySheet::OnCommand(WPARAM wParam, LPARAM lParam)
{
#ifdef _WIN32
	if (!m_bCommitingStart && (
		IDOK == LOWORD(wParam) && HIWORD(wParam) == BN_CLICKED))
#else
	if (!m_bCommitingStart && (IDOK == wParam && HIWORD(lParam) == BN_CLICKED))
#endif
	{
		if (GetActivePage() == m_pSearchGroupPage)
		{
			if (GetFocus() == m_pSearchGroupPage->GetDlgItem(IDC_EDIT_NEWSGROUP))
			{
#ifdef _WIN32
				m_pSearchGroupPage->SendMessage(WM_COMMAND, MAKELONG(IDC_SEARCH_NOW, BN_CLICKED),
									(LPARAM)(m_pSearchGroupPage->GetDlgItem(IDC_SEARCH_NOW)->GetSafeHwnd()));
#else
				m_pSearchGroupPage->SendMessage(WM_COMMAND, IDC_SEARCH_NOW, 
					MAKELONG(m_pSearchGroupPage->GetDlgItem(IDC_SEARCH_NOW)->GetSafeHwnd(), BN_CLICKED));
#endif
				return TRUE;
			}
		}
		if (XP_IsContextBusy(GetSubscribeContext()->GetContext()))
			XP_InterruptContext(GetSubscribeContext()->GetContext());
		((CSubscribePropertyPage*)GetActivePage())->EnableAllControls(FALSE);
		m_bCommitingStart = TRUE;
		MSG_SubscribeCommit(GetSubscribePane());
		return TRUE;
	}

	return CNetscapePropertySheet::OnCommand(wParam, lParam);
}

#ifdef _WIN32
 
BOOL CSubscribePropertySheet::OnInitDialog()
{
	BOOL ret = CNetscapePropertySheet::OnInitDialog();

	CreateProgressBar();

	return ret;
}

#else

int CSubscribePropertySheet::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int ret = CNetscapePropertySheet::OnCreate(lpCreateStruct);
	
	CreateProgressBar();

	return ret;
}

#endif

void CSubscribePropertySheet::CreateProgressBar()
{
	TEXTMETRIC tm;
  	CRect winRect, pageRect;

	HDC hDC = ::GetDC(GetSafeHwnd());
	GetTextMetrics(hDC, &tm);
	::ReleaseDC(GetSafeHwnd(), hDC);
	
	int nStatusHeight = tm.tmHeight + tm.tmInternalLeading+ tm.tmExternalLeading;

	GetWindowRect(winRect);
	SetWindowPos(NULL, 0, 0, winRect.right - winRect.left, 
		  winRect.bottom - winRect.top + nStatusHeight, SWP_NOMOVE);

	m_barStatus.Create(this, FALSE, FALSE);

	GetClientRect(winRect);
	m_barStatus.MoveWindow(0, winRect.bottom - winRect.top - nStatusHeight, 
		winRect.right - winRect.left, nStatusHeight, TRUE);
}

void CSubscribePropertySheet::SetStatusText(const char* pMessage)
{
	if (IsWindow(m_barStatus.GetSafeHwnd()))
		m_barStatus.SetPaneText(m_barStatus.CommandToIndex(ID_SEPARATOR), 
								pMessage, TRUE );
}

void CSubscribePropertySheet::Progress(const char *pMessage)
{
	if (IsWindow(m_barStatus.GetSafeHwnd()))
		m_barStatus.SetPaneText(m_barStatus.CommandToIndex(ID_SEPARATOR), 
								pMessage, TRUE );
}

void CSubscribePropertySheet::SetProgressBarPercent(int32 lPercent)
{
	if (::IsWindow(m_barStatus.GetSafeHwnd()))
		m_barStatus.SetPercentDone( lPercent );
}  

void CSubscribePropertySheet::StartAnimation()
{
	if (::IsWindow(m_barStatus.GetSafeHwnd()))
	{
		m_barStatus.StartAnimation();
	}
}

void CSubscribePropertySheet::StopAnimation()
{
	if (::IsWindow(m_barStatus.GetSafeHwnd()))
	{
		m_barStatus.StopAnimation();
	}
}

void CSubscribePropertySheet::AllConnectionsComplete(MWContext *pContext )
{
	if (m_bCommitingStart)
	{
		SendMessage(WM_COMMAND, IDOK, 0);
		return;
	}
	SendMessageToDescendants(WM_IDLEUPDATECMDUI, (WPARAM)TRUE, (LPARAM)0);

	StopAnimation();
	CSubscribePropertyPage* pPage = (CSubscribePropertyPage*)GetActivePage();
	if (pPage)
	{
		CNewsgroupsOutliner* pOutliner = pPage->GetOutliner();
		if (pOutliner)
		{
			if (pOutliner->SelectInitialItem())
			{
				int count;
				MSG_ViewIndex *indices;
				MSG_GroupNameLine group;

				pOutliner->GetSelection(indices, count);
				if (MSG_GetGroupNameLineByIndex(GetSubscribePane(), indices[0], 
					                            1, &group))
					pPage->CheckSubscribeButton(&group);
			}
		}

		if (pPage->GetMode() == MSG_SubscribeNew)
		{
			StopAnimation();
			((CNewNewsgroupsPage*)pPage)->m_bGetNew = FALSE;
			pPage->GetDlgItem(IDC_GET_NEW)->EnableWindow(TRUE);
			if (pPage->GetOutliner()->GetTotalLines())
				pPage->GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(TRUE);
			else
				pPage->GetDlgItem(IDC_CLEAR_NEW)->EnableWindow(FALSE);
			pPage->GetDlgItem(IDC_STOP)->EnableWindow(FALSE);
		}
	}
}

void CSubscribePropertySheet::AddServer(MSG_Host* pHost)
{
    if (::IsWindow(m_pSearchGroupPage->GetSafeHwnd()))
	{
		int nIndex = m_pSearchGroupPage->GetServerCombo()->AddString(MSG_GetHostUIName(pHost));
		if (nIndex != CB_ERR)
			m_pSearchGroupPage->GetServerCombo()->SetItemDataPtr(nIndex, pHost);
	}

    if (::IsWindow(m_pNewGroupPage->GetSafeHwnd()))
	{
		int nIndex = m_pNewGroupPage->GetServerCombo()->AddString(MSG_GetHostUIName(pHost));
		if (nIndex != CB_ERR)
			m_pNewGroupPage->GetServerCombo()->SetItemDataPtr(nIndex, pHost);
	}
}

void CSubscribePropertySheet::EnableNonImapPages(BOOL bEnable)
{
	if (::IsWindow(m_pSearchGroupPage->GetSafeHwnd()))
		m_pSearchGroupPage->EnableAllControls(bEnable);
    if (::IsWindow(m_pNewGroupPage->GetSafeHwnd()))
		m_pNewGroupPage->EnableAllControls(bEnable);
}

void CSubscribePropertySheet::OnHelp()
{
	if (GetActivePage() == m_pAllGroupPage)
		NetHelp(HELP_SUBSCRIBE_LIST_ALL);
	else if (GetActivePage() == m_pSearchGroupPage)
		NetHelp(HELP_SUBSCRIBE_SEARCH);
	else if (GetActivePage() == m_pNewGroupPage)
		NetHelp(HELP_SUBSCRIBE_LIST_NEW);
}

BEGIN_MESSAGE_MAP(CSubscribePropertySheet, CNetscapePropertySheet)
#ifndef _WIN32
    ON_WM_CREATE()
#endif
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CServerTypeDialog	
//	   
CServerTypeDialog::CServerTypeDialog(CWnd *pParent)
	: CDialog(CServerTypeDialog::IDD, pParent)
{
	m_pHost = NULL;
}

BOOL CServerTypeDialog::OnInitDialog()
{
	CheckDlgButton(IDC_RADIO_NNTP, TRUE);
	return CDialog::OnInitDialog();
}

void CServerTypeDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

void CServerTypeDialog::OnOK() 
{
	CDialog::OnOK();

	if (IsDlgButtonChecked(IDC_RADIO_NNTP))
	{
		m_pHost = DoAddNewsServer(GetParent(), FROM_SUBSCRIBEUI);
	}
	else if (IsDlgButtonChecked(IDC_RADIO_IMAP))
	{
		m_pHost = DoAddIMAPServer(GetParent(), NULL, TRUE);
	}
}

BEGIN_MESSAGE_MAP(CServerTypeDialog, CDialog)
	ON_BN_CLICKED(IDOK, OnOK)
END_MESSAGE_MAP()

