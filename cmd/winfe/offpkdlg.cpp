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
// offpkdlg.cpp : Offline picker dialog implementation file
//

#include "stdafx.h"
#include "msgcom.h"
#include "mailmisc.h"
#include "xp_time.h"
#include "xplocale.h"
#include "wfemsg.h"
#include "dateedit.h"
#include "nethelp.h"
#include "prefapi.h"
#include "nethelp.h"
#include "xp_help.h"
#include "offpkdlg.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


#define LEVEL_ROOT		0
#define LEVEL_1			1

extern "C" BOOL IsNumeric(char* pStr);
extern "C" void HelperInitFonts( HDC hdc , HFONT *phFont, HFONT *phBoldFont);




/////////////////////////////////////////////////////////////////////////////
// COfflinePickerCX
COfflinePickerCX::COfflinePickerCX(ContextType type, CDlgOfflinePicker *pDialog)  
: CStubsCX(type, MWContextMail)
{
	m_pDialog = (CDlgOfflinePicker*)pDialog;
}


/////////////////////////////////////////////////////////////////////////////
// COfflineList
STDMETHODIMP COfflineList::QueryInterface(REFIID refiid, LPVOID * ppv)
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

STDMETHODIMP_(ULONG) COfflineList::AddRef(void)
{
	return ++m_ulRefCount;
}

STDMETHODIMP_(ULONG) COfflineList::Release(void)
{
	ULONG ulRef;
	ulRef = --m_ulRefCount;
	if (m_ulRefCount == 0) 
		delete this;   	
	return ulRef;   	
}

void COfflineList::ListChangeStarting( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pDialog) 
		m_pDialog->ListChangeStarting( pane, asynchronous,
												 notify, where, num );
}

void COfflineList::ListChangeFinished( MSG_Pane* pane, XP_Bool asynchronous,
									   MSG_NOTIFY_CODE notify, MSG_ViewIndex where,
									   int32 num)
{
	if (m_pDialog) 
		m_pDialog->ListChangeFinished( pane, asynchronous,
												 notify, where, num );
}

void COfflineList::GetSelection( MSG_Pane* pane, MSG_ViewIndex **indices, int *count, 
							    int *focus)
{
}

void COfflineList::SelectItem( MSG_Pane* pane, int item )
{
}

//////////////////////////////////////////////////////////////////////////////
// COfflinePickerOutlinerParent

BEGIN_MESSAGE_MAP(COfflinePickerOutlinerParent, COutlinerParent)
END_MESSAGE_MAP()

COfflinePickerOutlinerParent::COfflinePickerOutlinerParent()
{
}


COfflinePickerOutlinerParent::~COfflinePickerOutlinerParent()
{
}

// Draw Column text and Sort indicator
BOOL COfflinePickerOutlinerParent::RenderData (int idColumn, CRect & rect, CDC &dc, const char * text)
{
	COfflinePickerOutliner* pOutliner = (COfflinePickerOutliner*) m_pOutliner;

	// Draw Text String
	if (idColumn == ID_COL_DOWNLOAD)
		dc.DrawText(text,  _tcslen(text), &rect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	else
		dc.DrawText(text,  _tcslen(text), &rect, DT_SINGLELINE | DT_VCENTER);

    return TRUE;
}


COutliner * COfflinePickerOutlinerParent::GetOutliner ( void )
{
    return new COfflinePickerOutliner;
}

void COfflinePickerOutlinerParent::CreateColumns ( void )
{
	CString text, newName; 
	CRect colRect;
	int col1, col2, nPos;
	int pos1, pos2 ;

	col2 = m_pDialog->nameWidth;
	col1 = m_pDialog->downloadWidth;
	pos2 = m_pDialog->namePos;
	pos1 = m_pDialog->downloadPos;
	

	text.LoadString(IDS_DOWNLOAD);	
	nPos = text.Find('&');
	if (nPos >= 0)
		newName = text.Left(nPos) + text.Right(text.GetLength() - nPos - 1);
	else
		newName = text;	
    m_pOutliner->AddColumn (newName, ID_COL_DOWNLOAD,	col1, colRect.right, 
							ColumnVariable, col1, FALSE);
 
	text.LoadString(IDS_DOWNLOAD_LIST);
	m_pOutliner->AddColumn (text, ID_COL_NAME,		col2, colRect.right, 
							ColumnVariable, col2, FALSE);

	m_pOutliner->SetColumnPos( ID_COL_DOWNLOAD, pos1 );
	m_pOutliner->SetColumnPos( ID_COL_NAME, pos2 );

	m_pOutliner->SetImageColumn( ID_COL_NAME );
}


//////////////////////////////////////////////////////////////////////////////
// COfflinePickerOutliner

COfflinePickerOutliner::COfflinePickerOutliner()
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
	m_pDialog = NULL;

}

COfflinePickerOutliner::~COfflinePickerOutliner ( )
{ 
	if (m_pUnkUserImage) {
        if (m_pIUserImage)
            m_pUnkUserImage->Release();
    }

	delete [] m_pAncestor;
	delete [] m_pszExtraText;

}


int COfflinePickerOutliner::GetDepth( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.level - 1;
}

int COfflinePickerOutliner::GetNumChildren( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.numChildren;
}

BOOL COfflinePickerOutliner::IsCollapsed( int iLine )
{
	MSG_FolderLine folderLine;
	MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) iLine, 1, &folderLine );
	return folderLine.flags & MSG_FOLDER_FLAG_ELIDED ? TRUE : FALSE;
}

BOOL COfflinePickerOutliner::RenderData(UINT iColumn, CRect &rect, CDC &dc, const char * text )
{
    if (iColumn != ID_COL_DOWNLOAD)
		return FALSE;

	if ((m_FolderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST)   ||
		(m_FolderLine.flags & MSG_FOLDER_FLAG_IMAP_SERVER) ||
		((m_FolderLine.flags & MSG_FOLDER_FLAG_MAIL) &&
		 !(m_FolderLine.flags & MSG_FOLDER_FLAG_IMAPBOX)))
		return TRUE;

	int idxImage = -1;

	if (MSG_GetFolderPrefFlags(m_FolderLine.id) & MSG_FOLDER_PREF_OFFLINE) 
		idxImage = IDX_CHECKMARK; 
	else
		idxImage = IDX_CHECKBOX; 

	m_pIUserImage->DrawImage(idxImage, rect.left + ( ( rect.Width ( ) - 16 ) / 2 ),
		                     rect.top, &dc, FALSE);
	return TRUE;
}


int COfflinePickerOutliner::TranslateIcon(void * pLineData)
{
    ASSERT(pLineData);
    MSG_FolderLine * pFolder = (MSG_FolderLine*)pLineData;

	BOOL bOpen = pFolder->numChildren <= 0 ||
				 pFolder->flags & MSG_FOLDER_FLAG_ELIDED ? FALSE : TRUE;

	return WFE_MSGTranslateFolderIcon( pFolder->level, pFolder->flags, bOpen );
}

int COfflinePickerOutliner::TranslateIconFolder (void * pData)
{
    ASSERT(pData);
    MSG_FolderLine * pFolder = (MSG_FolderLine*)pData;

	if (pFolder->numChildren > 0) {
		if (pFolder->flags & MSG_FOLDER_FLAG_ELIDED) {
            return(OUTLINER_CLOSEDFOLDER);
		} else {
            return(OUTLINER_OPENFOLDER);
		}
	}
    return (OUTLINER_ITEM);
}

BOOL COfflinePickerOutliner::ColumnCommand(int iColumn, int iLine)
{
	if (iColumn == ID_COL_DOWNLOAD)
	{
		//MSG_ViewIndex indices;
		//indices = (MSG_ViewIndex)iLine;
		MSG_GetFolderLineByIndex(m_pPane, iLine, 1, &m_FolderLine );
		if ( MSG_GetFolderPrefFlags(m_FolderLine.id) & MSG_FOLDER_PREF_OFFLINE )
		{
			MSG_SetFolderPrefFlags(m_FolderLine.id, !MSG_FOLDER_PREF_OFFLINE);
		}
		else
		{
			MSG_SetFolderPrefFlags(m_FolderLine.id, MSG_FOLDER_PREF_OFFLINE);
		}
		//MSG_Command(GetPane(), MSG_ToggleSubscribed, &indices, 1);
	}
    return FALSE;
}


void* COfflinePickerOutliner::AquireLineData(int line)
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

    m_pszExtraText[ 0 ] = '\0';
	if ( line >= m_iTotalLines)
		return NULL;
	if ( !MSG_GetFolderLineByIndex(m_pPane, line, 1, &m_FolderLine ))
        return NULL;

	return &m_FolderLine;
}


void COfflinePickerOutliner::GetTreeInfo( int iLine, uint32 * pFlags, int* pDepth, 
									   OutlinerAncestorInfo** pAncestor)
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;

	if (pAncestor) {
		if ( m_FolderLine.level > 0 ) {
			m_pAncestor = new OutlinerAncestorInfo[m_FolderLine.level];
		} else {
			m_pAncestor = new OutlinerAncestorInfo[1];
		}

		int i = m_FolderLine.level - 1;
		int idx = iLine + 1;
		while ( i > 0 ) {
			int level;
			if ( idx < m_iTotalLines ) {
				level = MSG_GetFolderLevelByIndex( m_pPane, idx );
				if ( (level - 1) == i ) {
					m_pAncestor[i].has_prev = TRUE;
					m_pAncestor[i].has_next = TRUE;
					i--;
					idx++;
				} else if ( (level - 1) < i ) {
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
		m_pAncestor[0].has_prev = FALSE;
		m_pAncestor[0].has_next = FALSE;

        *pAncestor = m_pAncestor;
	}

    if ( pFlags ) *pFlags = m_FolderLine.flags;
    if ( pDepth ) *pDepth = m_FolderLine.level - 1;
}


void COfflinePickerOutliner::ReleaseLineData(void *)
{
	delete [] m_pAncestor;
	m_pAncestor = NULL;
}


LPCTSTR COfflinePickerOutliner::GetColumnText(UINT iColumn, void* pLineData)
{
	MSG_FolderLine* pfolderLine = (MSG_FolderLine*)pLineData;

	memset(m_pszExtraText, '\0',  256);
	switch (iColumn) 
	{
		case ID_COL_NAME:
			strncpy(m_pszExtraText, pfolderLine->name, strlen(pfolderLine->name));
			break;
		default:
			break;
	} 
	return m_pszExtraText;
}																	

void COfflinePickerOutliner::OnSelChanged()
{
}

void COfflinePickerOutliner::OnSelDblClk()
{
}

int COfflinePickerOutliner::ToggleExpansion(int iLine)
{
	return  CMailNewsOutliner::ToggleExpansion(iLine);
}

// To select the first item in the outliner,
// so a user can tab through
BOOL COfflinePickerOutliner::SelectInitialItem()
{
	int count;
	MSG_ViewIndex *indices;
	GetSelection( indices, count );

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

/*
 *  Returns true if there are items in this outliner that can be
 *  selected for download.  False otherwise.
 */
BOOL COfflinePickerOutliner::HasSelectableItems()
{

	int numLines = GetTotalLines();
	MSG_FolderLine folderLine;

	for(int i = 0; i < numLines; i++)
	{

		if(MSG_GetFolderLineByIndex( m_pPane, (MSG_ViewIndex) i, 1, &folderLine ))
		{
			// As long as we're not a server or a non IMAP mailbox then
			// there is something to download.
			if (!((folderLine.flags & MSG_FOLDER_FLAG_NEWS_HOST)   ||
			(folderLine.flags & MSG_FOLDER_FLAG_IMAP_SERVER) ||
			((folderLine.flags & MSG_FOLDER_FLAG_MAIL) &&
			 !(folderLine.flags & MSG_FOLDER_FLAG_IMAPBOX))))
			{
				return TRUE;
			}

		}


	}
	return FALSE;

}

/////////////////////////////////////////////////////////////////////////////
// CDlgOfflinePicker dialog


CDlgOfflinePicker::CDlgOfflinePicker(CWnd* pParent /*=NULL*/)
	: CDialog(CDlgOfflinePicker::IDD, pParent)
{
	//{{AFX_DATA_INIT(CDlgOfflinePicker)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_nDiscussionSelectionCount  =0;
	m_nMailSelectionCount        =0;
	m_nDirectorySelectionCount   =0;
	m_nPublicFolderSelectionCount=0;
	m_pMaster = WFE_MSGGetMaster();

	m_pParent = pParent;
    m_bActivated = FALSE;
	m_bSelChanged = FALSE;
	m_bNotifyAll = FALSE;
	m_bDoShowWindow = TRUE;

	m_pOutliner = NULL;
	m_pOutlinerParent = NULL;

	m_bProcessGetDeletion = FALSE;
	m_bListChangeStarting = FALSE;
	nameWidth = 0;
	downloadWidth = 0;
	namePos = 1;
	downloadPos = 0;


}

CDlgOfflinePicker::~CDlgOfflinePicker()
{

}

void CDlgOfflinePicker::ListChangeStarting(MSG_Pane* pane, XP_Bool asynchronous,
	MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
	if ( pane == GetPane() ) 
	{
		m_bListChangeStarting = TRUE;
		
		if (m_pOutliner) 
			m_pOutliner->MysticStuffStarting(asynchronous, notify, where, num );
	}
}


void CDlgOfflinePicker::ListChangeFinished(MSG_Pane* pane, XP_Bool asynchronous,
	MSG_NOTIFY_CODE notify, MSG_ViewIndex where, int32 num)
{
	if ( pane == GetPane() )
	{
		m_bListChangeStarting = FALSE;
		if (m_pOutliner)
		{
			m_pOutliner->MysticStuffFinishing(asynchronous, notify, where, num);
			m_pOutliner->SetTotalLines(CASTINT(MSG_GetNumLines(GetPane())));
		}
	}
}


void CDlgOfflinePicker::CleanupOnClose()
{
	COfflineList* pList = GetList();
	if (pList)
	{
		pList->Release();
		SetList(NULL);
	}

	COfflinePickerCX* pCX = GetPickerContext();
	if (pCX)
	{
		if(!pCX->IsDestroyed())  
			pCX->DestroyContext();
		SetPickerContext(NULL);
	}
	MSG_Pane * pPane = GetPane();
	if (pPane)
	{
		MSG_DestroyPane(pPane);	
		SetPane(NULL);
	}
	
	if(m_pOutlinerParent)
	{
		delete m_pOutlinerParent;
	}

}


void CDlgOfflinePicker::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CDlgOfflinePicker)
		// NOTE: the ClassWizard will add DDX and DDV calls here
	//}}AFX_DATA_MAP
}


BEGIN_MESSAGE_MAP(CDlgOfflinePicker, CDialog)
	//{{AFX_MSG_MAP(CDlgOfflinePicker)
	ON_BN_CLICKED(IDC_OFFLINE_SELECT_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CDlgOfflinePicker message handlers


void CDlgOfflinePicker::OnOK() 
{                   	
	CDialog::OnOK();
	CleanupOnClose();
}

int CDlgOfflinePicker::DoModal ()
{
	if (!m_MNResourceSwitcher.Initialize())
		return -1;
	return CDialog::DoModal();
}


BOOL CDlgOfflinePicker::OnInitDialog() 
{
	CDialog::OnInitDialog();
	m_MNResourceSwitcher.Reset();

	m_iIndex = 0;

//We need to get a pane and context in order to get updates
	COfflinePickerCX* pCX = new COfflinePickerCX(MailCX,this);

	pCX->GetContext()->fancyFTP = TRUE;
	pCX->GetContext()->fancyNews = TRUE;
	pCX->GetContext()->intrupt = FALSE;
	pCX->GetContext()->reSize = FALSE;
	pCX->GetContext()->type = MWContextMail;
	SetPickerContext(pCX);

 	m_pPane = MSG_CreateFolderPane(pCX->GetContext(), m_pMaster);

	if (!m_pPane)
		return FALSE;
	
	//use  COfflineList to hook up with the backend
	COfflineList *pInstance = new COfflineList(this);
	SetList(pInstance);
	COfflineList** hList = GetListHandle();
	pInstance->QueryInterface(IID_IMsgList, (LPVOID *)hList);
	MSG_SetFEData((MSG_Pane*)m_pPane, (void *)pInstance);
//Done hooking up the pane and context to the list 

//Get the temporary box size for the outliner on the dialog!!!
  	CRect rect2, rect3, rect4;
	CWnd* widget = GetDlgItem(IDC_TEMP_OUTLINER_BOX);
	widget->ShowWindow( SW_HIDE );
	widget->GetWindowRect(&rect2);
	widget->GetClientRect(&rect3);
	GetClientRect(&rect4);
	ClientToScreen(&rect4);
	rect2.OffsetRect(-rect4.left, -rect4.top);

	downloadWidth = 60;
	nameWidth = rect2.Width() -downloadWidth;

	// do this here so that the resource dll switching can take place
	m_pOutlinerParent = new COfflinePickerOutlinerParent();

#ifdef _WIN32
	m_pOutlinerParent->CreateEx ( WS_EX_CLIENTEDGE, NULL, _T("NSOutlinerParent"), 
							WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_TABSTOP,
							rect2.left, rect2.top, 
							rect2.right - rect2.left, rect2.bottom - rect2.top,
							this->m_hWnd, NULL);
#else
	m_pOutlinerParent->Create( NULL, _T("NSOutlinerParent"), 
							 WS_VISIBLE|WS_CHILD|WS_CLIPCHILDREN|WS_TABSTOP,
							 rect2, this, NULL);
#endif

	m_pOutlinerParent->SetDialogOwner((CDlgOfflinePicker*)this);
	m_pOutlinerParent->CreateColumns();
	m_pOutliner = (COfflinePickerOutliner *) m_pOutlinerParent->m_pOutliner;
	if (m_pOutliner)
	{
		m_pOutliner->SetPane(m_pPane);
		m_pOutliner->SetTotalLines(CASTINT(MSG_GetNumLines(GetPane())));
		m_pOutliner->SetDlg(this);
		m_pOutliner->SelectInitialItem();
	}

	if (!m_pOutliner->HasSelectableItems())
		AfxMessageBox(IDS_NOTHING_SUBSCRIBED);


	return TRUE;
}

void CDlgOfflinePicker::OnCancel() 
{
	CDialog::OnCancel();
	CleanupOnClose();
}

void CDlgOfflinePicker::OnHelp() 
{
	NetHelp(HELP_MAILNEWS_SELECT_ITEMS);
}
