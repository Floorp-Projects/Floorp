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

// dlgseldg.cpp : implementation file
//

#include "stdafx.h"
#include "mailmisc.h"
#include "xp_time.h"
#include "xplocale.h"
#include "wfemsg.h"
#include "dateedit.h"
#include "nethelp.h"
#include "dlgseldg.h"
#include "prefapi.h"
#include "nethelp.h"
#include "xp_help.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

extern "C" void HelperInitFonts( HDC hdc , HFONT *phFont, HFONT *phBoldFont);

CDiscussionsList::CDiscussionsList(): CMailFolderList()
{
	m_iPosIndex = 0;
	m_iPosName = 50;
	m_iPosStatus = 100;
	m_pDWordArray = new CDWordArray;  //holds a list of groups with selection values.
	m_bHasSelectables= FALSE; //Flag that tells us if anything can be selected 

}

CDiscussionsList::~CDiscussionsList()
{
	FolderData *pData = NULL;

	while (-1 != m_pDWordArray->GetUpperBound())
	{
		pData = (FolderData*)m_pDWordArray->GetAt(m_pDWordArray->GetUpperBound());
		m_pDWordArray->RemoveAt(m_pDWordArray->GetUpperBound());
		m_pDWordArray->FreeExtra();
		if (pData)
		{
			delete pData;
			pData = NULL;
		}
	}
	if (m_pDWordArray)
		delete m_pDWordArray;
}

void CDiscussionsList::SetColumnPositions(int iPosIndex, int iPosName, int iPosStatus)
{
	m_iPosIndex = iPosIndex;
	m_iPosName = iPosName;
	m_iPosStatus = iPosStatus;
}

BEGIN_MESSAGE_MAP( CDiscussionsList, CMailFolderList )
    ON_WM_LBUTTONDOWN()
END_MESSAGE_MAP( )


int  CDiscussionsList::PopulateNews(MSG_Master *pMaster, BOOL bRoots)
{
	m_pMaster = pMaster;
	int index = 0;
	int nCount=0;
	m_iInitialDepth = 1;

	::SendMessage( m_hWnd, m_nResetContent, (WPARAM) 0, (LPARAM) 0 );

	int32 iLines = MSG_GetFolderChildren (m_pMaster, NULL, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];

	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (m_pMaster, NULL, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (m_pMaster, ppFolderInfo[i], &folderLine)) 
			{
				if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP ||
					 folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST || 
					 folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX   ||
					 folderLine.flags & MSG_FOLDER_FLAG_CAT_CONTAINER) 
				{
					if ( bRoots )
					{
						FolderData *pFolderData = new FolderData;
						if (pFolderData)
						{
							pFolderData->infoData = folderLine.id;
							pFolderData->bDownLoad = (BOOL)(MSG_GetFolderPrefFlags(folderLine.id) & MSG_FOLDER_PREF_OFFLINE);
							::SendMessage( m_hWnd, m_nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
							::SendMessage( m_hWnd, m_nSetItemData, (WPARAM) index,(DWORD)(pFolderData) );
							TRY
							{
								m_pDWordArray->Add((DWORD)pFolderData);
							}
							CATCH(CMemoryException, e)
							{
								delete [] ppFolderInfo;
								return  0;
							}
							END_CATCH
							index++;
						}
						if ( folderLine.numChildren > 0 ) 
						{
							SubPopulate(index, folderLine.id );
						}
					}
				}
			}
		}
		delete [] ppFolderInfo;
	}

	return 1;	
}


void CDiscussionsList::SubPopulate(int &index, MSG_FolderInfo *folder )
{
	int32 iLines = MSG_GetFolderChildren (m_pMaster, folder, NULL, 0);
	MSG_FolderInfo **ppFolderInfo = new MSG_FolderInfo *[iLines];

	ASSERT(ppFolderInfo);
	if (ppFolderInfo)
	{
		MSG_GetFolderChildren (m_pMaster, folder, ppFolderInfo, iLines);
		for (int i = 0; i < iLines; i++)
		{
			MSG_FolderLine folderLine;
			if (MSG_GetFolderLineById (m_pMaster, ppFolderInfo[i], &folderLine)) 
			{
					m_bHasSelectables = TRUE;
					FolderData *pFolderData = new FolderData;
					if (pFolderData)
					{
						pFolderData->infoData = folderLine.id;
						pFolderData->bDownLoad = (BOOL)(MSG_GetFolderPrefFlags(folderLine.id) & MSG_FOLDER_PREF_OFFLINE);
						::SendMessage( m_hWnd, m_nAddString, (WPARAM) 0, (LPARAM) folderLine.name );
						::SendMessage( m_hWnd, m_nSetItemData, (WPARAM) index,(DWORD)(pFolderData) );
						TRY
						{
							m_pDWordArray->Add((DWORD)pFolderData);
						}
						CATCH(CMemoryException, e)
						{
							delete [] ppFolderInfo;
							return;
						}
						END_CATCH
						index++;
					}
					if ( folderLine.numChildren > 0 ) {
						SubPopulate(index, folderLine.id);
					}
			}
		}
		delete [] ppFolderInfo;
	}
}



void CDiscussionsList::DrawItem( LPDRAWITEMSTRUCT lpDrawItemStruct )
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
	} else {
		hBrushFill = hBrushWindow;
		::SetBkColor( hDC, GetSysColor( COLOR_WINDOW ) );
		::SetTextColor( hDC, GetSysColor( COLOR_WINDOWTEXT ) );
	}

	VERIFY(::FillRect( hDC, &rcItem, hBrushFill ));

	if ( lpDrawItemStruct->itemID != -1 &&  dwItemData )
	{
		FolderData *  itemInfo = (FolderData *) dwItemData;
		MSG_FolderLine folderLine;
		MSG_GetFolderLineById( m_pMaster, (MSG_FolderInfo*)itemInfo->infoData , &folderLine);

		int idxImage = WFE_MSGTranslateFolderIcon( folderLine.level,
												   folderLine.flags, 
												   folderLine.numChildren );

		if ( m_hBoldFont && folderLine.unseen > 0 ) 
		{
			::SelectObject( hDC, (HFONT)&m_hBoldFont );
		}
		int iIndent = 4;

		BOOL bStatic = FALSE;
#ifdef _WIN32
	if ( sysInfo.m_bWin4 )
		bStatic = ( lpDrawItemStruct->itemState & ODS_COMBOBOXEDIT ) ? TRUE : FALSE;
	else 
#endif
		bStatic = m_bStaticCtl;

		if (!bStatic)
			iIndent += (folderLine.level - m_iInitialDepth) * 8;

//Draw the news bitmap
        m_pIImageMap->DrawImage( idxImage, iIndent, rcItem.top, hDC, FALSE );
		LPCTSTR name = (LPCTSTR) folderLine.prettyName;
		if ( !name || !name[0] )
			name = folderLine.name;
//Draw the text
		::DrawText( hDC, name, -1, &rcTemp, DT_SINGLELINE|DT_CALCRECT|DT_NOPREFIX );
		int iWidth = rcTemp.right - rcTemp.left;

		rcTemp = rcItem;
		rcText = rcItem;
		rcTemp.left = iIndent + 20;
		rcTemp.right = rcTemp.left + iWidth + 4;

		VERIFY(::FillRect( hDC, &rcTemp, hBrushFill ));
		rcText.left = rcTemp.left + 2;
		rcText.right = rcTemp.right - 2;
		::DrawText( hDC, name, -1, &rcText, DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX );
//handle the checkmark if it's a (news group or IMAP mail folder or a category container) and not a host
		if ((folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP || 
			folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX   ||
			folderLine.flags & MSG_FOLDER_FLAG_CAT_CONTAINER) &&
			!(folderLine.flags & MSG_FOLDER_FLAG_IMAP_SERVER))
		{
			BOOL bEnabled = itemInfo->bDownLoad;
			m_pIImageMap->DrawTransImage( bEnabled ? IDX_CHECKMARK : IDX_CHECKBOX,
										  m_iPosStatus, rcItem.top, hDC );
		}
//Draw the focus
		if ( lpDrawItemStruct->itemAction & ODA_FOCUS && 
			 lpDrawItemStruct->itemState & ODS_SELECTED )
		{
			::DrawFocusRect( hDC, &lpDrawItemStruct->rcItem );
		}	
	}

	if ( hBrushHigh ) 
		VERIFY( ::DeleteObject( hBrushHigh ));
	if ( hBrushWindow ) 
		VERIFY( ::DeleteObject( hBrushWindow ));

	if ( hOldFont )
		::SelectObject( hDC, hOldFont );
}


UINT CDiscussionsList::ItemFromPoint(CPoint pt, BOOL& bOutside) const
{
	RECT rect;
	GetClientRect(&rect);

	int iHeight = GetItemHeight(0);
	int iCount = GetCount();
	int iTopIndex = GetTopIndex();

	int iListHeight = iHeight * ( iCount - iTopIndex );
	rect.bottom = rect.bottom < iListHeight ? rect.bottom : iListHeight;

	bOutside = !::PtInRect(&rect, pt);

	if ( bOutside ) {
		return 0;
	} 

	return (pt.y / iHeight) + iTopIndex; 
}

void CDiscussionsList::OnLButtonDown( UINT nFlags, CPoint point )
{
	if ( point.x > m_iPosStatus ) {
		BOOL bOutside;
		UINT item = ItemFromPoint( point, bOutside );
		RECT rcItem;
		GetItemRect( item, &rcItem );

		if ( (point.y < rcItem.bottom) && (point.y >= rcItem.top) ) {
			FolderData *pItemData = (FolderData *)GetItemData( item );
			if ( pItemData ) {
				pItemData->bDownLoad = !pItemData->bDownLoad;
				InvalidateRect( &rcItem );
			}
		}
	} else {
		CListBox::OnLButtonDown( nFlags, point );
	}
}

/////////////////////////////////////////////////////////////////////////////
// CDlgSelectGroups dialog


CDlgSelectGroups::CDlgSelectGroups(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgSelectGroups::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgSelectGroups)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_nDiscussionSelectionCount=0;
	m_nMailSelectionCount=0;
}


void CDlgSelectGroups::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgSelectGroups)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgSelectGroups, CDialog)
	//{{AFX_MSG_MAP(CDlgSelectGroups)
	ON_BN_CLICKED(IDC_BUTTON_SELECT_ALL, OnButtonSelectAll)
	ON_BN_CLICKED(IDC_HELP_SELECT_FOR_DOWNLOAD, OnHelp)

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgSelectGroups message handlers

void CDlgSelectGroups::OnButtonSelectAll() 
{

	CListBox *pList = (CListBox*)GetDlgItem(IDC_LIST_DISCUSSIONS);
	if (pList)
	{
		int nCount = pList->GetCount();
		CRect rect;
		MSG_FolderLine folderLine;
		MSG_Master *pMaster = WFE_MSGGetMaster();
		for (int i = 0; i < nCount ; i++)
		{
			FolderData *pData = (FolderData*)pList->GetItemData(i);
			if (pData)
			{   //Make sure we are setting the properties of a group, not a host
				MSG_GetFolderLineById( pMaster, pData->infoData, &folderLine );
				if ( folderLine.flags & MSG_FOLDER_FLAG_NEWSGROUP || 
					folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX    ||
					folderLine.flags & MSG_FOLDER_FLAG_CAT_CONTAINER)  
				{
					pData->bDownLoad=TRUE;
					pList->SetItemData(i,(DWORD)pData);
					pList->GetItemRect(i,&rect);
					pList->InvalidateRect(rect);
				}
			}
		}

	}
}

void CDlgSelectGroups::OnOK() 
{                   
	MSG_FolderLine folderLine;
	MSG_Master *pMaster = WFE_MSGGetMaster();

	for (int i=0 ; i <= m_DiscussionList.m_pDWordArray->GetUpperBound(); i++)
	{
		FolderData *pData = (FolderData*)m_DiscussionList.m_pDWordArray->GetAt(i);
		if (pData)
		{   
			if (pData->bDownLoad)
			{
				MSG_SetFolderPrefFlags(pData->infoData, MSG_FOLDER_PREF_OFFLINE);
				MSG_GetFolderLineById( pMaster, pData->infoData, &folderLine );
				if ( folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX) 
					++m_nMailSelectionCount; //count the selected mail folders
				else
					++m_nDiscussionSelectionCount;     //count the selected discussion groups
			}
			else
				MSG_SetFolderPrefFlags(pData->infoData, !MSG_FOLDER_PREF_OFFLINE);
		}
	}

	PREF_SetIntPref("mail.selection.count",(int32)m_nMailSelectionCount);
	PREF_SetIntPref("offline.news.discussions_count",(int32)m_nDiscussionSelectionCount);
	
	CDialog::OnOK();
}


BOOL CDlgSelectGroups::OnInitDialog() 
{
	CDialog::OnInitDialog();

	m_DiscussionList.SubclassDlgItem( IDC_LIST_DISCUSSIONS, this );

	m_iIndex = 0;

	int iPosIndex, iPosName, iPosStatus;
	int iListLeft = 4;
	RECT rect, rcText;
	
	CWnd *widget = GetDlgItem(IDC_LIST_DISCUSSIONS);
	widget->GetWindowRect(&rect);

	::SetRect( &rcText, 0, 0, 64, 64 );

	iPosIndex = iListLeft + 4;
	iPosName = iPosIndex + rcText.right * 3;
	iPosStatus = rect.right - rect.left - 20 - GetSystemMetrics(SM_CXVSCROLL);

	m_DiscussionList.SetColumnPositions( iPosIndex, iPosName, iPosStatus );

	m_DiscussionList.PopulateNews(WFE_MSGGetMaster(),TRUE);

	if (!m_DiscussionList.m_bHasSelectables)
		AfxMessageBox(IDS_NOTHING_SUBSCRIBED);

	return TRUE;
}

void CDlgSelectGroups::OnCancel() 
{
	BOOL bEnabled = FALSE;
	MSG_FolderLine folderLine;
	MSG_Master *pMaster = WFE_MSGGetMaster();

#ifdef WE_USE_SELECTION_COUNT // currently not using this, so don't calculate it
	for (int i=0 ; i <= m_DiscussionList.m_pDWordArray->GetUpperBound(); i++)
	{
		FolderData *pData = (FolderData*)m_DiscussionList.m_pDWordArray->GetAt(i);
		BOOL bEnabled = (BOOL)(MSG_GetFolderPrefFlags(pData->infoData) & MSG_FOLDER_PREF_OFFLINE);
		if (bEnabled)
		{
				MSG_GetFolderLineById( pMaster, pData->infoData, &folderLine );
				if ( folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX) 
					++m_nMailSelectionCount; //count the selected mail folders
				else
					++m_nDiscussionSelectionCount;     //count the selected discussion groups
		}
	}
#endif WE_USE_SELECTION_COUNT
	CDialog::OnCancel();
}

void CDlgSelectGroups::OnHelp() 
{
	NetHelp(HELP_OFFLINE_DISCUSSION_GROUPS);
}
